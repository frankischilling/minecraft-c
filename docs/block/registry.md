# Block class construction

`mc_registry` executes the Block construction portion of the paired original
`uu`/`na` static initializer. It creates nine sounds, eight distinct property
arrays and 96 concrete Blocks in source order. This owner remains incomplete:
initialization returns `MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY` at bytecode PC3398,
before the first unfinished Item active use. It retains the reached storage in
the FAILED phase. It cannot provide a successfully initialized gameplay registry.

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

Superclass and subtype initialization have separate phases. Actual direction,
slab-name and repeater arrays, the empty leaf list, pressure-category identities
and sign Class identity are retained. Wire construction creates its own typed
empty Set/Map owners. Their later collection operations, tile construction and
gameplay methods remain unfinished.

Dispenser, chest and the two furnaces each construct a distinct ordinary Random
through the borrowed profile. Earlier global Block publication and tile flags
precede each private Random allocation. Seeds use the real profile clock path;
the owner supplies no fixed seed or entropy callback. These four constructions
do not initialize the separate Math Random holder.

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

`block.registry_construction` passes 101,839 checks on MSVC, GCC and Clang,
alongside `block.material` and `core.java_random_profile`. Linux runs use
ASan/UBSan and leak checks. Both client and server constructor paths are covered.
Checks include all 96 named/ID aliases, base fields and property slots, subtype
identities, separate owners, retained retries and five natural allocation-budget
failure boundaries. The byte-budget searches exercise real allocations; they
do not inject constructor failures or reset the shared profile.

The separate [executed-original comparison](../reference/block-registry.md)
matches all 3,950 projected base-field/array/reference rows per side on those
three compilers. It excludes concrete subtype state, clock-dependent values
and full bootstrap completion. The Item,
statistic, achievement, resource, recipe, smelting and final-hook closure remains
unfinished. Full registry startup, World integration, synchronous block hooks,
terrain-feature population, entity/tile behavior and gameplay persistence remain
required. See the [parity matrix](../parity.md) and [test scope](../testing.md).
