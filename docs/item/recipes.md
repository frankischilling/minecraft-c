# Crafting and smelting initialization

The [recipe API](../../include/mc/item/recipes/recipes.h) constructs the crafting
and smelting managers in the shared Block, Item, statistics and achievement
class owner. With resources supplied, the represented Statistics-first and
Block-first routes complete this initialization. Other entry orders retain
the original recursive failures described below. Recipe matching, inventory
results, furnace ticks, UI, World/population integration and gameplay
persistence remain unfinished.

## Construction and order

The crafting constructor runs seven helper families, then its 57 explicit
registrations. Together they create 151 recipes: 120 shaped and 31 shapeless.
Every registration evaluates the current Item and Block references in source
order and constructs real [ItemStack owners](stack.md). Output counts, damage,
pattern spaces and repeated references remain part of the constructed state.
Reached Stack allocations precede the field loads supplying their constructor
arguments; some registrations reuse an earlier output Stack. The implementation
does not load a final recipe-ID table.

Shaped registration accepts the reached String-array or consecutive-String
patterns and Character mappings. Each row sets the current width; the original
does not validate rectangularity. A mapped Item becomes a count-one,
damage-zero Stack; a mapped Block uses damage -1. Each occupied cell receives
a fresh copy of the mapped Stack. Unmapped cells stay NULL, while a present
mapping with a NULL value fails at the copy call. The shaped owner retains
width, height, the complete physical ingredient array, the output identity and
the output ID cached by its constructor. That cached ID remains separate from
the output Stack's current fields.

Shapeless registration constructs an ordered ingredient list. Stack arguments
are copied; Item and Block arguments create count-one, damage-zero Stacks.
Unsupported or NULL ingredient arguments return the native illegal-argument
category at the original throw. Earlier recipes and completed allocations
remain retained. A shapeless owner stores its output reference directly,
including NULL.

The stable crafting order places shaped recipes before shapeless recipes,
then orders each kind by decreasing size. Equal sizes keep registration order;
there is no output-ID tie-break. Native stable insertion sort implements this
pure comparator contract. It does not reproduce JDK TimSort allocation or its
sequence of comparator calls. The constructor prints the live list size with
the ` recipes` suffix before publishing its singleton.

Smelting executes ten registrations. Each evaluates the input Item or Block
ID before allocating and constructing its output Stack. Replacing a key
retains the supplied output identity rather than cloning it. Inspection and
statistics traversal use the retained JDK Integer-key HashMap bucket/next
order, not key sorting or registration order. The shared collection kernel
supplies that order; native value associations retain the actual Stack
references.

## Recursive initialization and failures

Both manager initializers publish their singleton at PC7, after the constructor
call at PC4 returns. An admitted recursive use can therefore observe NULL
while the real constructor is still running. Public reentry reports BUSY.
Inspectors read existing state without initializing or repairing a class.

| First explicit entry, with resources supplied | Observed boundary on both original sides |
| --- | --- |
| Statistics | Ordinary class initialization returns. Both managers publish, all sixteen achievements exist, and final G/H are false. |
| Block | Ordinary class initialization returns. Both managers publish, all sixteen achievements exist, and final G/H are true. |
| Item | The first achievement's real RAW Stack reaches its NULL Item ID read at PC2. Both manager singletons remain NULL. |
| Crafting | Recursive statistics reads the still-NULL crafting singleton and fails at `jl/gg.d` PC24. Both manager singletons remain NULL. |
| Smelting | Crafting has published, but recursive statistics reads the still-NULL smelting singleton and fails at `jl/gg.d` PC75. |

Failed native classes cache their first cause and preserve completed writes,
including unpublished constructor prefixes. A later public call reports
FAILED_INITIALIZATION with that saved cause; it does not replay construction.
The enclosing initializer position and the innermost source-operation
diagnostic remain separate. Missing resources and native allocation limits can
stop earlier than the finite original boundaries in the table.

The source anchors are client/server crafting `hk/ey`, smelting `ey/de`,
shaped `is/ft`, shapeless `tt/ml`, comparator `ky/gx` and statistics `jl/gg`.
Crafting sorts at PC3201 and prints at PC3234. Statistics calls the managers at
PC21/72 and dereferences their returned singletons at PC24/75. The
[catalog provenance](../../tests/reference/item/recipes/provenance.json) binds
the exact original JAR hashes, JDK inputs, observer sources and fixture bytes.

## Inspection and lifetime

The public interface exposes manager phases, crafting-list order, shaped
fields, shapeless ingredients, output references and smelting lookup/order.
Shaped-array extent is native inspection metadata, separate from the original
width and height fields. Smelting key export allocates nothing; insufficient
capacity leaves the output storage and count unchanged. A missing smelting key
succeeds with NULL, as does a stored NULL value.

Manager, recipe, list and array views borrow the common class lifetime. Each
Stack is an actual independently held owner, including RAW owners left by
failed construction. Retain a Stack explicitly before using it after class
retirement. Registry copies create a fresh identity with the source ID, count
and damage, animation zero and no NBT provenance; they do not apply registry,
stack-size or durability validation. The class owner releases its Stack holds
before freeing its allocation ledger.

Native bounds cover collection sizes, checked array arithmetic and the shared
allocation budget, including external Stack storage and retained failed work.
Typed transient operands and arena allocation differ from Java heap objects.
These limits and failure positions do not reproduce Java OutOfMemoryError
timing.

## Evidence and validation

The [original observer](../../tests/reference/statistics/ArrayPrefixController.java)
captured both untouched original JARs for all five entries in interpreted,
tier-1 and default Temurin 17.0.17+10 modes: 30 captures, equal within each
side/entry across modes. It reads published managers, every recipe and
ingredient cell, smelting bucket/next order, and each distinct Stack's four
fields. Stable reference labels preserve observed aliases. Unpublished
constructor objects and the rest of the heap are outside the projection.

For Statistics and Block, the completion marker follows the ordinary
`Class.forName` return. Every debuggee process still uses requested debugger
termination. Released mirrors, reaped children and unchanged inputs are
recorded; natural process completion is not claimed. The 36 legacy and
achievement replay captures remain byte-equal to their historical fixtures.

The [catalog comparator](../../tests/tools/item/compare_recipes.py),
[numeric graph comparator](../../tests/tools/statistics/compare_arrays.py) and
[Stack-value comparator](../../tests/tools/item/compare_stack_values.py) consume
separate original-derived fixtures. Native synthetic display resources affect
omitted text. Actual console payloads are compared with only the explicitly
identified OS line separator adapted. The
[Stack projection](../../tests/reference/item/stack_values/provenance.json)
covers twelve explicit constructor inputs and their actual copy fields; its
original-only animation mutation is excluded from the native comparison.

Final native qualification passes 30 selected Windows cases and 32 per Linux
compiler, with ASan/UBSan and leak detection on Linux. Contracts report 2,872
recipe checks, 556 registry-Stack checks, 11,312,084 array checks and 32,892
achievement checks. All ten numeric graphs, ten catalogs and both Stack
projections match on MSVC, GCC and Clang with imported and synthetic resources.
The graph row counts per side are 1,705/1,705/413/1,338/1,338 for Statistics,
Block, Item, crafting and smelting. All 555 code/tool/fixture inputs agree
before/after and across retained host source copies; 32 products per host are
retained. clang-format18 passes 280 C/header files. These comparisons do not
qualify crafting gameplay or full World startup.
