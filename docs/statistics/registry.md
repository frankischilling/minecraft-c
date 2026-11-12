# Statistics construction and registration

The [statistics API](../../include/mc/statistics/registry/registry.h) shares the
Block, Item, Translation and achievement-key class owner. Base `vr/ns` constructs
its integer formatter with `Locale.US` and its decimal formatter with the native
format locale. The represented numeric profiles are English (US) and English
without a country. Other successfully queried profiles fail explicitly at the
locale-dependent constructor. Linux queries use a private locale and preserve
the fresh standalone JVM's C-locale fallback when environment setup fails.
They do not change the process-global locale or reproduce an embedding host's
preexisting JVM locale.

The two number owners retain numeric configuration and strings. Three distinct
strategy owners retain integer, time and distance identity. Runtime number
formatting remains unfinished; constructor configuration alone does not qualify
its rounding, floating-point conversion or formatted output.

Base, general and item-stat constructors retain their actual ID, display,
formatter and flag fields. Item stats also retain the associated item ID.
Constructors do not register. Registration rejects every occupied ID, including
the same object, and retains the duplicate message as UTF-16. Its spaces come
from the original constant pool, including the space before the numeric ID.

Successful base registration appends the object to list `b`, inserts it in map
`a`, then calls the actual [key loader](keys.md) before assigning `h`. General
registration appends to list `c` only after that sequence returns. Missing keys
are allowed NULL values. Failed key initialization retains earlier list/map
publication and its original nested cause. Native diagnostic map lookup does
not expose or promise Java HashMap iteration order.

`jl/gg` initialization creates four distinct lists and registers all 23 common
statistics in source order. Display arguments use the real
[translation classes](../resources/translation.md). Block and Item initialization
now reach this actual class initialization at their original calls. Public
reentry reports BUSY; an admitted synchronous recursive use observes the same
partial storage. Each failed class caches its first failure without rollback.
Inspection does not make a failed class usable.

The block-statistics helper at PC618 now walks the live Block array, checks
each actual admission flag, translates its name and registers item-stat owners.
The item-use and item-break helpers walk the live Item array and apply the
actual durability predicate. Their formatted names use the reached
[String/null formatter](../reference/string-format.md). Registration precedes
array publication and list insertion. Unification preserves aliases, removes
the first object from the three reached lists by identity, and retains its ID
map entry. The original self-pair also removes its list entry.

The recipe helper `d()` runs only when both `G/H` are true. It collects current
crafting outputs, then current smelting values, in a real allocator-backed
Integer HashSet. It reads each output Stack's live ID, rather than the shaped
recipe's cached ID. Only after both traversals does it publish the 32,000-slot
`D` array at PC129. Each craft stat uses a fresh live Item lookup and formatted
name, registers before its array store, and retains the array captured before
construction. Final unification reloads the current `D`. No once flag or
rollback is added.

With resources supplied, Statistics-first and Block-first complete the
[recipe and smelting construction](../item/recipes.md) and all sixteen
[achievements](achievements.md). Both complete graphs contain 125 nonnull `D`
slots and 121 crafting-statistic objects in the ID map and list `b`. Statistics-first ends with `G/H` false;
Block-first ends with both true because its later Item/Block callbacks set
them again. These flags describe source-ordered state, not permanent readiness.
Item-first still fails at the first icon Stack's NULL Item read at PC2,
retaining `C` and earlier writes. Direct crafting and smelting entry fail at
the still-NULL manager receiver in `d()` at PC24 and PC75 respectively.
World/population integration, synchronous world mutations and gameplay
persistence acceptance remain unfinished.

The [native unit](../../tests/statistics/registry/test_registry.c) uses synthetic
resources to check publication order, aliases, formatter configuration, failure
prefixes, duplicate messages, retries, reentry and natural allocation budgets.
Its isolated registration controls explicitly model an in-progress initializer;
they do not manufacture a completed original class. Original resource text
remains private. The additional
[array unit](../../tests/statistics/registry/test_arrays.c) checks all three
entry orders, unification, partial publication, formatting failures, retained
identities, natural budgets and allocation-free cached retries. It verifies
the exact advancement of the five actual ordinary Random owners.

The [original observer](../../tests/reference/statistics/RegistryObservation.java)
normally initializes untouched `jl/gg` in an isolated class loader. Both original
JARs complete in interpreted, tier-1 and default Temurin 17.0.17+10 modes. The
original `151 recipes` and `16 achievements` output is retained unchanged before
the observer protocol. Reflection checks each common stat's exact class,
distinct identity, map/list membership and selected live strategy reference.
The comparison covers 25 protocol rows per side: header, 23 full field rows and
footer. It does not compare the rest of the original initialized registry.

The current [bootstrap fixtures](../../tests/reference/statistics/bootstrap/provenance.json)
cover both original sides and five explicit entries in interpreted, tier-1 and
default modes: 30 equal-per-route captures. The
[array controller](../../tests/reference/statistics/ArrayPrefixController.java)
observes ordinary `Class.forName` return for Statistics and Block, or the
actual original NULL dereferences at PC2/24/75 for the other entries. Every
debuggee process still uses requested debugger termination; returned class
initialization does not establish natural process completion. The numeric
graphs contain 1,705/1,705/413/1,338/1,338 rows per side in Statistics, Block,
Item, crafting and smelting order. They omit resource text and unpublished
partial heap objects.

The recipe checkpoint's qualification passes 30 selected Windows cases and 32 per Linux
compiler, with Linux ASan/UBSan and leak detection. All ten graphs, ten recipe
catalogs and both Stack-value projections match on MSVC, GCC and Clang.
The units report 14,566 statistics and 11,312,084 array checks; each Linux
locale case adds one statistics check. See
the [recipe evidence](../item/recipes.md#evidence-and-validation) for the
catalog and Stack scope. Thirty-six new legacy/achievement replay captures
also match the unchanged historical fixtures.

The later shared-World-registry regression passes all 22 selected cases on
MSVC/GCC/Clang, including these graph replays. The array unit now reports
11,312,060 checks because its native allocation-budget search includes the new
registry borrow counter. The earlier counts above describe the recipe
checkpoint; this change adds no new original statistics observation.

At the preceding array checkpoint, the same controller observed untouched
original initialization at exact recipe/achievement call
boundaries. Both sides and all three entry orders agree across interpreted,
tier-1 and default modes: 18 actual captures. The helper verifies its original
class loader; JDI reads fields and arrays without invoking suspended game
methods. Complete numeric graph projections contain 953 rows for Stats-first
and 396 rows for each other entry. Requested debugger termination, mirror
release and child reaping are recorded separately from natural completion.
The retained [array fixtures](../../tests/reference/statistics/arrays/provenance.json)
record that historical boundary, including recipe d21; they do not describe
the current implementation's stopping point. All six numeric graphs matched
on MSVC, GCC and Clang using synthetic display resources and every projected
reference; that observation omitted display and key text. That checkpoint's
selected run passed 23 Windows cases and 25 cases per Linux compiler with
ASan/UBSan and leak detection. Native contracts reported
14,564 statistics and 3,841,912 array checks; each Linux locale variant added
one statistics check. The allocator extension also preserves the existing
captured JDK HashSet histories and survives failure at every allocation
request in its finite ownership test.

At the earlier common-statistics checkpoint, the
[native file probe](../../tests/statistics/registry/files_probe.c), consuming
the privately imported original bytes, matched that projection on MSVC, GCC and
Clang. At that checkpoint its native class failed explicitly at PC618. The unit
passed 12,918 checks per compiler; Linux's explicit `C.UTF-8` and invalid-category
cases each passed 12,919 with ASan/UBSan and leak detection. All 15 selected
Windows cases and 17 selected Linux cases passed, retaining the earlier Reader, Properties,
Translation, key, Block and Item comparisons. The invalid-category regression
also covers glibc's cached lookup failure without a fresh `errno` value.
