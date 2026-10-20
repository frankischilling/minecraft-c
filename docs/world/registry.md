# Shared registry access from Worlds

A GAME World class can bind the application's existing Block/Item registry
before its first World allocation. The side and Random-profile address must
match. Binding retains one native borrow without initializing Block, reading
resources, drawing randomness or changing materials. Every World of that class
uses the same owner. Another matching World class can share it.

The class keeps this borrow until all its Worlds have been freed. RAW, failed
and retired Worlds still count; external Dimension, Entity or Tile bindings can
prolong their lifetime. Registry destruction returns BUSY while a borrow or an
initialization call remains. Materials, profile and resources retain their
separate dependency lifetimes. Binding cannot replace an owner or attach one
after the World class has admitted its first allocation.

Class-bound [Dimension Biome managers](biome-registry.md#world-and-dimension-managers)
retain additional independent Registry borrows. A separately held manager can
outlive its World and World class, so freeing those owners does not by itself
make Registry destruction possible. Manager retirement only releases its own
borrow; reference arrays in the Registry arena never retain that arena cyclically.

## Reached operations

The ordinary registry getters execute actual Block active use. A cold owner
runs its existing initializer; an erroneous owner propagates its cached cause.
These getters do not turn diagnostic failed-prefix storage into gameplay
values. Static reads can return a nullable cell. Array reads access the actual
`m` or `o` storage, checking its reference before the signed index. `o` is the
captured boolean array, distinct from a virtual material or Block predicate.

The internal captured `p` view retains the actual Tile-bearing flag array.
Its get performs ordinary Block active use and publishes no view if that use
fails. A later element read uses the captured reference, including a captured
NULL, without reloading the field or initializing the class again. NULL precedes
signed bounds; false is a successful element value. This implements the array
access at `lm.d(III)Low;` / `hi.d(III)Ljh;` PCs42/47. The later Tile lookup,
factory and Map/List ownership joins remain unfinished.

World material lookup acquires the block first. Air returns the same shared
`Material.a` without initializing Block. Nonair performs Block active use,
loads the actual `m` element and returns that Block's nullable `bA` material.
A missing Block receiver fails explicitly. The result is a shared object
identity; it is not a material enum derived from the numerical generator.

Dimension spawn predicates also query their held World's top block first:

- Overworld then reads actual `Block.F.bn` and compares the retained top ID.
- Nether reads `Block.A.bn`, rejects that ID and air, then reads live `o[top]`.
- Sky rejects air before Block active use. Otherwise it reads `m[top].bA` and
  invokes that same material's `c` predicate.

Missing registry capability fails only when the operation reaches it. A failed
top query therefore precedes a registry error; Sky air needs no Block access.
The complete native predicate/material call holds the actual World through
resource callbacks, so retirement and free remain BUSY during the call.
Ordinary outputs remain unchanged on native failure. Nested registry errors
retain their first initializer/constructor cause separately from the caller's
World or Dimension diagnostic.

The load/create spawn loop retains `y=true`, starts at `(0,64,0)`, and reloads
the live Dimension on every attempt. Each rejection draws four separate
`World.r.nextInt(64)` values: complete wrapped x arithmetic before drawing the
z pair. Acceptance changes the same Info's spawn coordinates before clearing
`y`. The loop has no retry cap, extra clock, reseed or rollback.

## Evidence and limits

The paired original bytecode supplies these operations:

| Operation | Client | Server | Bytecode |
| --- | --- | --- | --- |
| Spawn loop | `fd.c()V` | `dj.c()V` | Predicate18; draws31/40/53/62; Info setter78; clear83 |
| Base spawn predicate | `xa.a(II)Z` | `os.a(II)Z` | Top6; F11; bn14 |
| Nether predicate | `wd.a(II)Z` | `nz.a(II)Z` | Top6; A11; bn14; o28; index32 |
| Sky predicate | `ay.a(II)Z` | `aj.a(II)Z` | Top6; air15; m16; element20; bA21; c24 |
| World material | `fd.f(III)Lln;` | `dj.d(III)Lhj;` | Block4; air material14; m18; element23; bA24 |
| Info spawn setter | `ei.a(III)V` | `ct.a(III)V` | Scalar stores2/7/12 |

Archive identities and the existing executed registry comparisons are recorded
in the [Block reference](../reference/block-registry.md). The operations above
are source evidence, not a new executed-original World comparison.

The 150 constructed World/registry checks pass within 22 selected cases on
MSVC, GCC and Clang, with Linux ASan/UBSan and leak detection. They use real
direct-metadata Worlds and an explicitly restored, store-owned cache baseline.
Checks cover inert and mismatched bindings, sharing, RAW/failed/retired
lifetimes, air without Block initialization, ordinary nonair initialization,
all three predicates, missing capability/resource failures, cached first
causes and BUSY retirement/free during registry resource callbacks. They do
not execute fresh spawn acceptance or its rejection draws. The unchanged
constructor and recorded-observation regressions pass separately in that run.

Ordinary missing-chunk GAME generation still fails at its explicit registry
and lighting join. The [shared Biome module](biome-registry.md) now supplies
actual class owners and a direct sampler that can initialize Block during the
first Biome lookup, after the first climate-cell stores. World/Dimension now
binds actual retained normal/fixed managers. The live generator join remains
unfinished. [World height and lighting submission](runtime-lighting.md) use the
actual provider and C/A storage; live Chunk finishing, propagation, activation,
population, full block setters and gameplay persistence remain unfinished.
Prepared cache baselines and native constructor completion
cannot establish full fresh-world startup or original population parity.

See [construction](construction.md), [block access](blocks.md) and the
[parity matrix](../parity.md).
