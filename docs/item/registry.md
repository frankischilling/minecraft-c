# Item construction and recursive class initialization

`mc_registry` owns the actual Block and Item construction state in one lifetime
above Worlds. The [Item interface](../../include/mc/item/registry/registry.h)
uses the same owner as the [Block interface](../block/registry.md), borrowing
the application's live Material and Random profile owners. Allocation creates
COLD metadata. Each public initialization entry requires exclusive access and
the documented strict floating-point environment.

Its statistics invocation now enters actual
[common-stat construction and registration](../statistics/registry.md). Missing
native resource support retains the nested Translation constructor cause;
imported resources reach the explicit block-stat helper boundary at PC618.
The suspended Item and Block initializer positions remain separate from that
innermost failure.

## Construction and publication

Item initialization constructs its ordinary static Random through the borrowed
profile, publishes it, then allocates and publishes the actual 32,000-entry
Item reference array. It executes all 106 named constructor and property
chains in source order. The represented closure contains 38 concrete kinds,
their required ancestors, three effective-Block arrays, two armor arrays,
the dye arrays and five distinct tool-material enum owners.

Every Item retains the eight actual base fields. Same-letter subtype and
ancestor fields have separate storage. The base constructor publishes the
new Item into the current array before subtype construction, icon/name setters
and named publication. Later failures retain those earlier writes. Name
setters construct distinct owned UTF-16 strings; equal contents do not imply
equal identity. Literal operands retain their shared owner-local identities.
Container Items retain their actual referenced Item, with the reached
max-stack check before publication.

The first tool's declaring-class initialization occurs before raw allocation
and argument evaluation. Its effective-Block array reads the current Block
static fields, which can enter Block initialization. Each active use captures
its array operand only after initialization returns; a later getstatic reads
the live cell again. This preserves the real recursive initialization cycle.

## The two entry orders

| Explicit entry | Retained state at the current implementation boundary |
| --- | --- |
| Block first | All 96 Blocks and all 106 named Items exist. Item reaches its unfinished statistics invocation at PC2664; Block remains suspended at its Item-array active use, PC3398. No BlockItem tail or final Block flag write has run. |
| Item first | Item publishes its Random and array, then the first tool enters Block. Block constructs all 96 Blocks, seven explicit special BlockItems and the remaining default BlockItems, using that same partial Item array. The conditional fire hook runs and `r[0]` becomes true before the unfinished Block statistics invocation at PC3674. The 106 named Item chains have not begun. |

Both entries currently return `MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY` and retain
FAILED class state. Neither provides completed registry bootstrap. The actual
statistics, resource, achievement, recipe and smelting closure remains required.
An occupied Item slot also stops explicitly at the unfinished conflict-output
dependency; the original warning branch must eventually continue to its later
store rather than become a duplicate-ID rejection.

BlockItem constructors perform the actual virtual face-texture lookup on the
live Block receiver. The default-item loop invokes the post-Item virtual hook
only when the current Item slot was null. Fire fills its two private arrays in
the original order; it is not initialized by an unconditional final table.

## Failures, observations and lifetime

Each declaring class retains its own phase, source position and first failure.
Automatic frames preserve the suspended parent initializer and current object
context. Public reentry reports BUSY; only the admitted synchronous internal
flow can observe an INITIALIZING class's actual partial state. A failed class
cannot replay its initializer. Diagnostics retain Block's driver position and
the innermost active class/position separately.

The native allocation budget includes metadata, allocation records and every
retained allocation, including unpublished objects. A native limit is not a
claim about Java OutOfMemoryError timing. All objects and captured array views
remain live until quiescent owner destruction. The owner frees its ledger once;
the borrowed Material/profile owners and their committed effects survive.

Public observations read the live fields, named references and actual array
identity without triggering initialization. Reading a FAILED owner does not
make it READY. See the [original Item observations](../reference/item-registry.md)
for the finite comparison and its exclusions, and the [test record](../testing.md)
for executed validation. World binding, gameplay bodies and whole population
and persistence acceptance remain unfinished.
