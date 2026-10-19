# Block class construction

`mc_registry` executes the Block construction portion of the paired original
`uu`/`na` static initializer. It creates nine sounds, eight distinct property
arrays and 96 concrete Blocks in source order, followed by the real recursive
[Item construction](../item/registry.md). This owner remains incomplete:
Both entry orders execute actual
[statistics](../statistics/registry.md) and
[achievement initialization](../statistics/achievements.md). With imported
resources, Block-first completes nested Item/statistics/achievement work and
reaches the unfinished recipe call at d21. Item-first instead reaches the
first achievement icon while named Items are still NULL, failing on its real
ItemStack Item read at2. Both retain failed Block state. Without a resource
provider the earlier Translation resource-open dependency still stops them.
Neither supplies a complete gameplay registry.

The [public interface](../../include/mc/block/registry/registry.h) allocates one
class owner above Worlds, borrowing the application's actual Material and Random
profile owners. Allocation creates only COLD native metadata. Initialization
requires exclusive access and the documented strict floating-point environment.
No World, provider stream or replacement Material owner is created.

## Executed state and order

The implementation retains all 113 base static cells separately: nine sound
references, eight property-array references and 96 named Block references.
Every Block has the actual 17 base fields, separate declaring-class subtype
fields and its concrete constructor dispatch. Names are distinct owned UTF-16
strings; repeated literal operands share their owner-local literal identities.
Named statics and the ID array alias the same concrete objects.

The base constructor publishes `m[id]` before the ID field, bounds and two
virtual opacity calls. Those calls read the reached subtype fields. In
particular, leaves and both slabs retain their early constructor results;
later flag assignments do not replace them with final-object predicates.
Hardness, resistance, emission and bounds preserve the original separate float
operations and float-to-double widening.

Superclass and subtype initialization have separate phases, source positions
and retained first failures. Automatic frames preserve suspended parent state.
Actual direction,
slab-name and repeater arrays, the empty leaf list, pressure-category identities
and sign Class identity are retained. Wire construction creates its own typed
empty Set/Map owners. Their later collection operations, tile construction and
gameplay methods remain unfinished.

Dispenser, chest and the two furnaces each construct a distinct ordinary Random
through the borrowed profile. Earlier global Block publication and tile flags
precede each private Random allocation. Seeds use the real profile clock path;
the owner supplies no fixed seed or entropy callback. These four constructions
do not initialize the separate Math Random holder. Item constructs its fifth
ordinary Random through that same profile at its actual initialization point.

## Failure and lifetime

The native byte budget includes owner metadata, allocation records and all
retained allocations. A failed constructor or later fluent call preserves its
earlier object, array, static and profile effects. Named publication occurs only
after the corresponding chain succeeds. Repeated initialization reports
`MC_REGISTRY_FAILED_INITIALIZATION` without restarting or rolling back effects.
Native resource-limit diagnostics do not reproduce Java OOM scheduling.

Diagnostic views expose the reached live storage without Java active use.
Inspection after failure does not make initialization successful. Quiescent
destruction releases this owner's allocations, including unpublished failed
children; borrowed Material/profile owners and their effects survive. Views
must be retired before destruction.

## Native checks and remaining work

`block.registry_construction` passes 103,163 checks on MSVC, GCC and Clang,
within six selected registry, Material, profile and recorded-observation cases.
Linux runs use
ASan/UBSan and leak checks. Both client and server constructor paths are covered.
Checks include all 96 named/ID aliases, base fields and property slots, subtype
identities, separate owners, retained retries and six natural allocation-budget
failure boundaries. A nested direction-array failure retains its actual class
and PC49 even after a distinct Item initialization failure. The byte-budget searches exercise real allocations; they
do not inject constructor failures or reset the shared profile.

The separate [executed-original comparison](../reference/block-registry.md)
matches all 3,950 projected base-field/array/reference rows per side on those
three compilers. It excludes concrete subtype state, clock-dependent values
and full bootstrap completion. Item has its own 531,481 native checks and
[33,064-row original comparison](../reference/item-registry.md). The
block-statistic, achievement, recipe and smelting closure remains
unfinished. Full registry startup, World integration, synchronous block hooks,
terrain-feature population, entity/tile behavior and gameplay persistence remain
required. See the [parity matrix](../parity.md) and [test scope](../testing.md).
