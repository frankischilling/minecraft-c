# Original Item construction observations

Both untouched [pinned originals](binaries.md) were observed at Item initializer
PC2664, immediately before the statistics active use. Their measured stacks
retain the outer Block initializer at PC3398. The experiment explicitly enters
Block through one isolated original-JAR loader; it does not establish ordinary
application startup order or the separate Item-first path.

[The helper](../../tests/reference/item/ItemRegistryPrefixObservation.java)
checks the original JAR's size and SHA-256 before loading it with the platform
parent. [The controller](../../tests/reference/item/ItemRegistryPrefixController.java)
binds the actual loader, both initializer bytecodes and exact instruction
boundaries, then reads the suspended VM through JDI. It invokes no game methods,
writes no game fields and supplies no replacement classes, resources or World.

## Capture and termination

The public-tool capture contains 40,842 client rows and 40,741 server rows.
It records all 108 Item statics, 106 distinct named Items, eight base fields,
subtype and ancestor fields, tool/armor/dye state and every physical cell of
the actual 32,000-entry Item array. The related 96-Block graph and five distinct
ordinary Random owners remain in the raw data. Declaring-class metadata,
primitive bits, reference identities, UTF-16 contents, JDK state and the measured
stack are retained without replacing them with expected C values.

The public [provenance](../../tests/reference/item/registry-prefix/provenance.json)
binds 104 sanitized input fingerprints, seven compiled helper classes and the
complete artifact ledger. Its actual capture-manifest SHA-256 is
`2d927d98abb6b2e491cb45631d67ec4a7189f2b92cb721c50250537340a8aff2`.

After capture, the controller requests debugger `exit(0)`. Both owned children
exit zero and are reaped, with complete streams, no watchdog firing and
unchanged original/runtime/helper inputs. This is deliberate debugger
termination. Neither initializer completes naturally, and no completed
statistics or registry bootstrap follows from reaching this checkpoint.

The capture uses pinned Windows Temurin 17.0.17 in interpreted mode. The
capture driver binds source files, compiled helper classes, Java tools/modules,
native runtime libraries, originals, streams and process receipts. The larger
array is admitted explicitly with a 100,000-row, 65,536-cell individual-array,
262,144-cell total and 8 MiB TSV bound. An unavailable or partial observation
does not qualify original execution.

## Finite comparison

The [original-only projector](../../scripts/reference/item/project_registry_prefix.py)
and [public native probe](../../tests/item/registry/prefix_probe.c) compare
the eight base fields of all 106 Items, their named references, the actual
Item-array reference and all 32,000 cells, plus encountered name-String
identities and UTF-16 contents. Reference renumbering preserves equality and
inequality. Scalar bits, nulls, array order and String contents are unchanged.

All 33,064 rows per side match on Windows MSVC and Linux GCC/Clang, with Linux
ASan/UBSan and leak checks. Each projected fixture is 622,620 bytes. The native
probe reads actual public views and contains no expected property vectors.
The projector rejects three specific corruptions per side: missing final LF,
a duplicate instance-field row, and a missing physical array cell. Rebound
framing counters ensure the latter two reach their intended structural guards.
The first export attempt rejected the actual VM's signed synthetic-field
modifier values; the corrected parser admits canonical signed 32-bit modifiers
and preserves their full bits. Original snapshots and projected values were
unchanged by that parser correction.

This projection excludes subtype/class metadata, JDK internals, clock-dependent
Random/profile values, VM/thread/loader state and initialization phase.
The original remains suspended INITIALIZING; the incomplete C entry has stopped
FAILED at its explicitly unfinished dependency. Those phases are not equated.
Native subtype, entry-order and retained-failure checks are separate controls.
See the [native contract](../item/registry.md) for remaining implementation work.

The later [common-stat integration](../statistics/registry.md) preserves this
field projection while advancing the native failure into actual statistics
and Translation initialization. Its no-provider probe now requires the nested
`nh/ii` initializer PC4 and constructor PC23, retaining the outer Block3398 and
Item2664 positions. It does not equate that later failure with the original
debugger's suspended phase or broaden the recorded projection.

## Reproduction

Public replay requires neither Java nor original files. Local recapture requires
both verified originals and the pinned Windows JDK, using fresh output paths:

```powershell
python scripts/reference/item/capture_registry_prefix.py --java "$jdk/bin/java.exe" --javac "$jdk/bin/javac.exe" --client b1.7.3.jar --server b1.7.3_server.jar --output-dir .local/fresh-item-capture
python scripts/reference/item/project_registry_prefix.py --capture-dir .local/fresh-item-capture --output-dir .local/fresh-item-projection
```

Keep complete raw snapshots and lifecycle receipts with any exported fixture.
The earlier [Block-only observation](block-registry.md) retains its separate
checkpoint, capture tools, provenance and fixture unchanged.

## Held attack and renderer references

The later [held attack packet](../../tests/reference/entity/held_item_attack/README.md)
uses ordinary completed original App, World, Player and Stack constructors.
Its prepared warm registry has 202 nonnull Item IDs across 38 runtime Classes.
It records every actual Class's combat and rendering method owners, plus all
22 direct GM IDs, 20 tools/Swords, durability boundaries and Saddle cases.
The 422 deterministic rows retain the earlier 32 Dirt/Wool observations. Three
additional health-10 Cow deaths audit each process's actual private Random,
sound pitch, Leather quantity and ordered publication before comparing fixed
fields. Actual random outcomes remain recorded. The five unarmed rows are unchanged.

The [direct geometry packet](../../tests/reference/client/held_item_geometry/README.md)
records five Sword icons and Bow through genuine original App/RenderEngine/Font,
Player, Stack, RA and NW owners on a hidden Pbuffer. Ordinary/read-only-JDI
execution supplies 36 actual flush buffers and 19,008 exact words. The actual
Font-used initial buffer remains intact; captured inactive color/reserved slots
are zero. The full-word comparison has that finite prior-state scope. It does
not infer Sword observations from the older RAW-App Bow capture.

The [native combat contract](../item/combat.md) and
[first-person hand contract](../client/first-person-hand.md#held-item-leaves-and-extrusion)
separate these new component observations from the older suspended initializer
projection, complete graphical encounters, original pixels and hosted results.

## Damaged Item overlay reference

The [overlay fixture](../../tests/reference/client/item_durability_overlay/finite.json)
and [capture provenance](../../tests/reference/client/item_durability_overlay/provenance.json)
cover nine genuine original BB overlay calls. Eight damaged profiles complete
three rectangles each; the pristine profile draws none. The original supplies
24 actual NW draws, 1,152 raw words and 16 actual `Math.round(D)J` returns.
The native CPU comparison matches 576 active XYZ/color words from entered
rectangle prefixes with zero completed native draws. Each process separately
validates its own inactive prior backing. Raw stale Font UV words remain intact.

Read-only JDI pins these eight actually entered original methods against the
untouched archive:

| Client method | Observed role |
| --- | --- |
| `bb.b(Lsj;Lji;Liz;II)V` | Overlay predicate and damaged-Item body |
| `bb.a(Lnw;IIIII)V` | Private colored-rectangle helper |
| `nw.a()V` | Actual original draw |
| `iz.g()Z` | Damaged predicate and initial damage operand |
| `iz.e()Z` | Damageability lookup |
| `iz.h()I` | Independent raw damage reads |
| `iz.j()I` | Independent maximum-damage lookups |
| `gm.f()I` | Actual maximum field body |

The three-test Windows selection passes in 0.76 seconds, including eight packet
corruption controls. This selected CPU scope does not establish native completed
GL-draw, full HUD/frame or pixel equality. The separate frozen Windows SDK Dirt
and Iron-Sword-break scenarios now pass their native graphical paths and
exact-live-Inventory autosave audits. Their
[qualification record](../testing.md#held-item-attacks-and-direct-extrusion)
does not expand the original packet's finite projection.
