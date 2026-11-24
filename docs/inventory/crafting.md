# Crafting inventories and containers

The shared `mc_inventory` library owns InventoryPlayer, InventoryCrafting,
InventoryCraftResult and the concrete Player/Workbench containers. Recipe
matching executes the existing registry's sorted catalog of 151 recipes.
These objects are dependencies of real Player construction. They do not yet
provide inventory screens, slot clicks or crafting-output pickup.

## Construction and notification

InventoryPlayer owns 36 main slots, four armor slots and a separate cursor
occurrence. Its selected slot starts at zero and its changed flag starts false.
The constructor stores a borrowed Player identity without requiring that
Player's construction to have returned. The ordinary getter, setter, splitter,
size, name, limit and dirty methods preserve their original order. A setter
does not clamp counts or mark the inventory dirty.

InventoryCrafting owns its width, wrapped width-times-height slot array and
concrete Container back-reference. Coordinate reads check X first, then use
Java signed32 row arithmetic and the linear getter. Setters and nonempty splits
synchronously notify their concrete Container after mutation. A null handler
therefore fails after the reached store or split; it does not suppress the
notification or undo the preceding change.

InventoryCraftResult owns one slot. Its splitter returns the whole stored
occurrence regardless of the requested amount. Its setter does not notify a
Container. The grid and result inventory retain their genuine no-op dirty and
always-true usability bodies.

The Player container has 45 Slots: one crafting result, four crafting inputs,
four armor slots, 27 main slots and nine hotbar slots. The Workbench container
has 46 Slots, using nine crafting inputs and no armor slots. Each container
owns distinct slot, snapshot and listener Lists and a blocked-Player Set.
Crafting Slots retain their input-grid link; armor Slots retain their actual
container link. Constructors preserve the original indices and screen positions.
The Workbench stores the supplied World and coordinates without reading World
state during construction.

Both constructors call their actual inventory-change body. That body captures
the result receiver before CraftingManager active use, initializes the real
singleton, scans its actual recipe list and replaces result slot zero. An empty
grid follows this same path. Every later successful input mutation recomputes
the preview synchronously.

## Recipe matching and results

The catalog contains 120 shaped and 31 shapeless recipes. The manager scans its
retained sorted list and returns the first matching recipe's freshly allocated
result. It adds no later-version repair recipe.

Shaped matching visits X offsets before Y offsets, tries the mirrored layout
first and checks every cell in the virtual 3x3 area. Required empty cells matter.
It resolves the expected ingredient before reading the grid cell. Shapeless
matching reads the grid in row order and removes the first matching remaining
ingredient, preserving duplicate ingredients. Ingredient damage `-1` matches
any damage value. Counts and NBT do not affect matching; a nonnull Stack with a
zero or negative count still occupies its cell.

Each successful result copies ID, count and damage into a new Stack. Animation
starts at zero and native storage provenance is absent. Repeated preview
updates replace the result with a distinct object, even when its values match.

## Ownership and failures

Every stored Stack occurrence owns a hold. Public getters add an independent
hold; whole removals transfer the stored hold; split and recipe results transfer
their new owner's initial hold. Replacement releases the displaced occurrence.
Borrowed observations require a live owner and exclusive access.

A Container owns its grid, result, Slots and collection storage. Its grid's
handler is a borrowed back-reference. Grid/result retirement is busy while
their Container owns them. InventoryPlayer retirement is busy while a Container
borrows it. Registry classes, the Player and the Workbench World remain borrowed
and must outlive their dependents. Native retirement performs no source
close, drop, pickup or Player callback.

Source null, index and negative-array outcomes remain distinct from native
allocation, count and work limits. Diagnostics retain the reached source frames
and descendant registry/Stack cause. Input mutations preceding a notification
failure remain visible. Failed constructors do not publish an output owner.

The default inventory metadata budget is 16 MiB with 65,536 slots. Configurable
ceilings are 1 GiB and 1,048,576 slots. The metadata budget includes allocation
headers, arrays, collections and Slots; external Stack/provenance and borrowed
owners have separate budgets. Matching bounds scratch storage at 65,536
ingredients and 65,536 recipes, sharing 1,048,576 work steps across one manager
scan. These native limits do not claim JVM allocation-failure timing.

## Verification scope

The native tests exercise both source sides, every recipe, translated/mirrored
layouts, intruders, damage wildcards, duplicate ingredients, nonpositive counts,
unchanged inputs, fresh outputs, Slot graphs and retained failure effects.
Root Windows MSVC 19.51 builds pass 254 outer inventory/container assertions
per side, in addition to the matching suite. Sanitized Linux Clang 18.1.3 and
GCC 13.3 builds each pass 253 per side; native allocation-budget branches depend
on object layout. All three products match 1,153 original rows per source side
across 1,047 authored cases. The
[reproducible comparison](../../tests/reference/inventory/crafting/README.md)
invokes unchanged original methods in fresh JDK 17 processes, verifies class
origins and Code pins, and excludes Player/World initialization. Only platform
line endings are normalized. All three integrated tests pass per compiler;
Linux enables ASan, UBSan and leak checks.

The native probe uses a bounded byte-aware plan scanner. A regression preserves
the earlier reader's acceptance of a valid row followed by NUL and hidden data;
the corrected Windows and sanitized Linux products reject that input before
emitting a case row. Their ordinary original-row comparisons still pass.
The public protocol driver checks 48 accepted/rejected cases per compiler.
Rejections require exit 1, source trace diagnostics and the completed graph
prefix; sanitizer diagnostics cannot count as successful input rejection.

Full Player construction, inventory NBT/gameplay/armor methods, output pickup,
click/shift-click/merge, cursor hooks, close/drop, eligibility and statistics
remain unfinished. Map crafting output reaches actual World map-data creation;
that effect has no successful placeholder implementation here.
