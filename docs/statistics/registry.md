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

The next block-statistics helper at PC618 remains explicitly unfinished. The
initializer retains the preceding 23 registrations and the `stat.mineBlock`
literal but does not publish array `C`, initialize achievements or execute the
later `G/H` assignments. The item-array `b()/c()` bodies, achievements, recipes,
smelting and complete bootstrap still require implementation. This module does
not complete population, synchronous world mutation or persistence acceptance.

The [native unit](../../tests/statistics/registry/test_registry.c) uses synthetic
resources to check publication order, aliases, formatter configuration, failure
prefixes, duplicate messages, retries, reentry and natural allocation budgets.
Its isolated registration controls explicitly model an in-progress initializer;
they do not manufacture a completed original class. Original resource text and
raw observations remain private.

The [original observer](../../tests/reference/statistics/RegistryObservation.java)
normally initializes untouched `jl/gg` in an isolated class loader. Both original
JARs complete in interpreted, tier-1 and default Temurin 17.0.17+10 modes. The
original `151 recipes` and `16 achievements` output is retained unchanged before
the observer protocol. Reflection checks each common stat's exact class,
distinct identity, map/list membership and selected live strategy reference.
The comparison covers 25 protocol rows per side: header, 23 full field rows and
footer. It does not compare the rest of the original initialized registry.

The [native file probe](../../tests/statistics/registry/files_probe.c), consuming
the privately imported original bytes, matches that projection on MSVC, GCC and
Clang. Its class still fails explicitly at PC618. The native unit passes 12,918
checks per compiler; Linux's explicit `C.UTF-8` and invalid-category cases each
pass 12,919 with ASan/UBSan and leak detection. All 15 selected Windows cases and
17 selected Linux cases pass, retaining the earlier Reader, Properties,
Translation, key, Block and Item comparisons. The invalid-category regression
also covers glibc's cached lookup failure without a fresh `errno` value.
