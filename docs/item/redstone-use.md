# Redstone Item use

Item331 now executes the original `IE.a(ItemStack,Player,World,IIII)Z` body through
the retained ItemStack wrapper. Its placement check uses the separate
`SM.a(World,III)Z` support predicate. The placed Block55 enters the existing
[synchronous Redstone lifecycle](../world/redstone.md) and
[World renderer](../client/redstone-world.md).

## Method order

The body first reads the clicked Block. Clicking the live Snow ID selects the
same coordinates and bypasses the offset and air test. Otherwise, six independent
face comparisons adjust one coordinate with Java signed wrapping. An unknown
face leaves the coordinates unchanged. The actual target-air query then decides
whether use returns false.

The support predicate reads `World.h(x, y - 1, z)` with wrapped subtraction. It
does not reuse the general Block occupancy predicate or Torch's support rule.
An unsupported target still returns true from the Item body, without consuming
the Stack or calling the setter. The enclosing wrapper therefore reaches the
used statistic.

When support succeeds, the body subtracts one from the live Stack count before
calling the notifying World setter. It reloads the live Redstone singleton and
its Block ID after the count store. The ordinary Boolean setter result is
discarded: a false result still consumes one Item and returns true. A failed
callback preserves the completed count/Block/notification prefix and stops
before the wrapper statistic.

There is no positive-count check, remote guard, sound or durability call in this
body. Signed count subtraction wraps. The existing wrapper captures its used
statistic array and then reads the late Stack ID, so a publication callback that
changes the Stack to ID332 awards the ID332 statistic.

## Original and native replay

The [original packet](../../tests/reference/item/redstone_use/README.md) contains
42 genuine wrapper calls and 12 direct support calls. Each cohort runs in two
fresh ordinary VMs with a separate saved World. Rows within one cohort retain
their genuine World, registered Redstone owner, lighting queue and Set history.
The helper prepares declared arrays before each call; it never clears the
lighting queue or Set, reseeds a stream, or repairs a Source phase or clock.

The packet admits exact reached outcomes and actual Java exception frames.
Unrelated Throwable, setup errors, unexpected entities and daemon errors fail
admission. Its entropy records compare each World's own stream before and after
each call. They do not compare constructor seeds across processes.

`item.redstone_reference` uses actual saved Chunk loads, Registry, App, statistics
writer, Session, LocalPlayer and Stack owners. Its separate support process
constructs no Player or App. The comparison checks all sparse array changes,
ordered lighting regions and WorldAccess events, Stack fields, used statistics,
source flags, complete observed Set boundaries and modification history. The
captured Set stays empty while its modification count advances to 69.

The comparison covers 53 full outcomes and one callback effect prefix. For that
prefix, the original observer throws `IllegalStateException`; the native observer
returns a separately checked refusal. Their exception classes are not equated.
The Stack count99 and late ID332 callback cases use real field stores.

Local MSVC Release builds and Clang Debug builds with AddressSanitizer and
UndefinedBehaviorSanitizer pass the packet suite, 106 comparator corruption
controls and the complete native replay. Fourteen neighboring Item-use,
placement, Redstone lifecycle and renderer cases also pass on both builds.

Terminal native fixture cleanup starts after all observed methods and
WorldAccess removal. It captures descriptors before the terminal entity latch,
detaches cache references, retires each actual unique Store Chunk once, then frees
the owners. This cleanup neither saves nor resumes the World and does not process
the observed lighting queue or Redstone Set.

Run the asset-independent packet, comparator controls and native replay:

```sh
ctest --test-dir build --output-on-failure -R '^(tools[.]redstone_use_(packet|comparator)|item[.]redstone_reference)$'
```

## Windows SDK placement and persistence

The Windows Release scenario passes 3,051 native checks in 8.41 seconds. A single
SDL SDK right-button down/up pair reaches actual picking, controller, Item331
use and the notifying setter. The same held Stack changes from count2 to count1,
the used331 statistic advances once, and the target becomes Block55/metadata0.
No placement sound or Item entity is emitted. This is SDK event coverage, with
no physical hardware-input claim.

The prelaunch input contains 441 populated saved Chunks and an isolated Dirt/Air
target. Actual terrain lists retain their 60-vertex prefix and append 12 packed
Redstone vertices. Passive Store, Registry, World and dimension fields bracket
the placement compile; a Source brightness getter would itself mark a Chunk as
read. The captured operands independently determine light and binary32 color.
The original CPU floor row supplies topology and UVs. All XYZ, UV and color
words match. Each raw vertex retains 32 bytes; the comparison covers its first
24 bytes and leaves unused tail words outside the equality claim.

Ordinary Escape, Save and Quit and reselection preserve the Block and Inventory.
Independent Region and Player NBT reads agree with the actual paused save entry.
All 441 saved Chunks remain, with no Item entities. Reselection changes only the
eight-byte session token. The reloaded geometry matches the placed geometry;
its initial compile had no paired pre-frame operand observation and is recorded
as unpaired. No recompilation was forced to obtain that evidence.

The run checks unchanged source/product/imported-resource inputs, all 27
post-capture corruption controls, terminal owners and absent native windows.
Its native products and raw observations remain retained. A separate
`tools.redstone_placement_controls` test checks the parser and geometry with
declared examples and the original CPU row, without starting a client.

Enable `MC_ENABLE_CLIENT_REDSTONE_PLACEMENT_TESTS` together with the existing
display/imported-resource configuration, then run:

```sh
ctest --test-dir build-windows --output-on-failure -R '^client[.]redstone_placement_loop$'
```

This finite replay does not qualify powered circuits, arbitrary callback
schedules, populated Set trees, default equal-hash identity ordering, a loaded
original scene, whole-frame pixels, general focus reliability, Linux loaded play
or FPS.
